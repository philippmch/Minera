"""Round CB: the pandas sequence filter domain with datetime64 and integer
keys.

Loads scripts/pandas_sequence_filter_typed_domain.py and adds datetime64
case and activity columns (us naive, ns naive, us UTC, ms Europe/Berlin, with
NaT) and int64/Int64 activity columns (activities a, b, c, d and the odd ones
map to fixed values). On integer activity columns the activity arguments of
the calls take the mapped integers instead of the strings in about half of
the cases."""
import importlib.util, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("pandas_sequence_filter_typed_domain", HERE / "pandas_sequence_filter_typed_domain.py")
typed = importlib.util.module_from_spec(_spec)
sys.modules["pandas_sequence_filter_typed_domain"] = typed
_spec.loader.exec_module(typed)
base = typed.base
ORDER = ["a", "b", "c", "d", "", "ab", "a,b", "...", "é", "z"]
DT_KINDS = {"dt64": ("us", None), "dt64ns": ("ns", None), "dt64tz": ("us", "UTC"), "dt64ber": ("ms", "Europe/Berlin")}
CASE_KINDS = list(DT_KINDS)
ACT_KINDS = list(DT_KINDS) + ["int64", "Int64"]
VALUE_FUNCTIONS = {"start_activities.apply", "start_activities.filter_df_on_start_activities", "end_activities.apply",
                   "end_activities.filter_df_on_end_activities", "traces.apply", "starts_with.apply", "ends_with.apply",
                   "prefixes.apply", "suffixes.apply", "rework.apply", "activity_split.apply"}
_typed_spec = typed.frame_spec
def frame_spec(r):
    spec = _typed_spec(r)
    if r.random() < 0.5:
        spec["case_kind"] = r.choice(CASE_KINDS)
    if r.random() < 0.6:
        spec["act_kind"] = r.choice(ACT_KINDS)
    spec["int_args"] = r.random() < 0.5
    return spec
def _code(a):
    return None if a is None else (ORDER.index(a) + 1 if a in ORDER else 40)
def _stamps(pd, codes, kind, hours):
    unit, zone = DT_KINDS[kind]
    values = [pd.NaT if c is None else pd.Timestamp(2021, 1, c, c % 24) + pd.Timedelta(minutes=hours * c) for c in codes]
    s = pd.Series(values, dtype=f"datetime64[{unit}]")
    return s.dt.tz_localize(zone) if zone else s
_typed_case = typed._case_series
def case_series(spec, pd):
    if spec["case_kind"] in DT_KINDS:
        return _stamps(pd, spec["cases"], spec["case_kind"], 7)
    return _typed_case(spec, pd)
_typed_act = typed._act_series
def act_series(spec, pd):
    k = spec["act_kind"]
    codes = [_code(a) for a in spec["acts"]]
    if k in DT_KINDS:
        return _stamps(pd, codes, k, 0)
    if k == "int64":
        return pd.Series([0 if c is None else c for c in codes], dtype="int64")
    if k == "Int64":
        return pd.Series(pd.array(codes, dtype="Int64"))
    return _typed_act(spec, pd)
typed._case_series = case_series
typed._act_series = act_series
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
        if spec.get("act_kind") in ("int64", "Int64") and spec.get("int_args"):
            for call in case["calls"]:
                if call["function"] in VALUE_FUNCTIONS and call["args"] and "m" not in call["args"][0]:
                    call["args"][0] = _to_int(call["args"][0])
    return cases
_base_encode = base.encode_result
def encode_result(value):
    """A NaT dict key has no typed JSON: the call counts as unrepresentable."""
    try:
        return _base_encode(value)
    except ValueError as error:
        if "NaT" in str(error):
            raise TypeError("unrepresentable NaT") from error
        raise
base.encode_result = encode_result
base.frame_spec = frame_spec
typed.frame_spec = frame_spec
for _k in dir(typed):
    if not _k.startswith("__"): globals().setdefault(_k, getattr(typed, _k))
globals()["frame_spec"] = frame_spec
globals()["generate_cases"] = generate_cases
