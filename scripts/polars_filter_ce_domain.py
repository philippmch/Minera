"""Round CE: the Polars table filter domain with edge values on typed keys.

Loads scripts/polars_filter_cc_domain.py (typed case and activity columns)
and adds:

- Boolean and all-null (Null dtype) activity columns;
- an "edge" argument mode: activity values of the calls become values that
  do not fit the column or sit at the limits of the comparison model (ints
  past 2**53, at and above the Int64 range, ints that do not fit the narrow
  integer columns, floats past 1e16, NaN and the infinities, negative zero,
  datetimes before 1970, bools, unknown strs), alone or in lists and tuples;
- numeric filters (apply_numeric, apply_numeric_events) on the activity
  column with typed and edge bounds, and on any column with the activity as
  stream filter value.

Ints above the UInt64 range are not sent (the bank's typed JSON carries at
most UInt64). Duration, Time, Decimal and Int128 columns are not covered:
the table transport has no cell for their values."""
import datetime as _dt
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_filter_cc_domain", HERE / "polars_filter_cc_domain.py")
cc = importlib.util.module_from_spec(_spec)
sys.modules["polars_filter_cc_domain"] = cc
_spec.loader.exec_module(cc)
base = cc.base
codec = cc.codec
S, I, F, B, N = codec.S, codec.I, codec.F, codec.B, codec.N

NEW_KINDS = ["Boolean", "Null"]
EDGE_INTS = [2 ** 53 + 1, 2 ** 53, 2 ** 63 - 1, 2 ** 63, 2 ** 63 + 5, 2 ** 64 - 1, -1, -(2 ** 63), 0, 300, 255, 256,
             -129, 70000, 65536, 5_000_000_000, 4_294_967_296, -2_147_483_649, 1, 2, 3]
EDGE_FLOATS = [1e17, 1e16 + 2.0, float(2 ** 53 + 1), -0.0, 0.0, 2.5, float("nan"), float("inf"), float("-inf"),
               1e6, 1e7, 0.30000000000000004, 123456789.125, -1.5]
EDGE_DATES = [_dt.datetime(1969, 12, 31, 23, 59, 59, 999000), _dt.datetime(1960, 1, 1),
              _dt.datetime(1960, 7, 1, 12, 30, tzinfo=_dt.timezone.utc), _dt.datetime(1900, 1, 1, 0, 0, 0, 1),
              _dt.datetime(1970, 1, 1), _dt.datetime(2021, 1, 1, 1, 0, 0, 1000),
              _dt.datetime(2021, 1, 1, tzinfo=_dt.timezone(_dt.timedelta(hours=5, minutes=30)))]
EDGE_STRS = ["zz", "zz9", "", "a", "b"]

_cc_frame_spec = cc.frame_spec
_cc_act_series = cc.act_series
_cc_generate = cc.generate_cases


def frame_spec(r):
    spec = _cc_frame_spec(r)
    if r.random() < 0.22:
        spec["act_kind"] = r.choice(NEW_KINDS)
    if r.random() < 0.45:
        spec["arg_mode"] = "edge"
    return spec


def act_series(spec, pl, name):
    kind, acts = spec["act_kind"], spec["acts"]
    if kind == "Boolean":
        return pl.Series(name, [None if a is None else cc._code(a) % 2 == 1 for a in acts], dtype=pl.Boolean)
    if kind == "Null":
        return pl.Series(name, [None] * len(acts), dtype=pl.Null)
    return _cc_act_series(spec, pl, name)


def edge_value(r):
    pick = r.random()
    if pick < 0.3:
        return I(r.choice(EDGE_INTS))
    if pick < 0.5:
        return F(r.choice(EDGE_FLOATS))
    if pick < 0.7:
        return codec.wall(r.choice(EDGE_DATES))
    if pick < 0.8:
        return B(r.random() < 0.5)
    return S(r.choice(EDGE_STRS))


def edge(r, node):
    (tag, value), = node.items()
    if tag in ("l", "t"):
        items = [edge(r, v) for v in value]
        if r.random() < 0.2:
            items.append(edge_value(r))
        return {tag: items}
    if tag == "s" and r.random() < 0.55:
        return edge_value(r) if r.random() < 0.85 else {r.choice(("l", "t")): [edge_value(r) for _ in range(r.randint(0, 2))]}
    return node


def bound(r):
    pick = r.random()
    if pick < 0.12:
        return N()
    if pick < 0.2:
        return S(r.choice(EDGE_STRS))
    return edge_value(r) if r.random() < 0.6 else I(r.randint(0, 6))


def extra_calls(r, spec):
    """Numeric filters on the activity column and with activity stream values."""
    names = spec["names"]
    calls = []
    for fn in ("apply_numeric", "apply_numeric_events"):
        if r.random() < 0.7:
            entries = [[S("pm4py:param:attribute_key"), S(names["act"])]]
            if r.random() < 0.3:
                entries.append([S("positive"), B(r.random() < 0.5)])
            if fn == "apply_numeric" and r.random() < 0.5:
                entries.append([S("stream_filter_key1"), S(r.choice((names["act"], "org:resource", "rd")))])
                entries.append([S("stream_filter_value1"), bound(r)])
                if r.random() < 0.4:
                    entries.append([S("stream_filter_key2"), S(r.choice((names["act"], "rd:kind")))])
                    entries.append([S("stream_filter_value2"), bound(r)])
            if names["case"] != "case:concept:name":
                entries.append([S("pm4py:param:case_id_key"), S(names["case"])])
            r.shuffle(entries)
            calls.append({"function": "attributes." + fn, "args": [bound(r), bound(r), {"m": entries}], "kwargs": []})
    return calls


def generate_cases(count, bank_seed):
    cases = _cc_generate(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xCE)
    for case in cases:
        spec = case["spec"]
        r = random.Random(rng.randrange(1 << 30))
        if spec["arg_mode"] == "edge":
            for call in case["calls"]:
                for position in cc.FUNCTIONS.get(call["function"], []):
                    if position < len(call["args"]) and "m" not in call["args"][position]:
                        call["args"][position] = edge(r, call["args"][position])
                if call["function"].startswith("attributes.apply_numeric"):
                    last = call["args"][-1] if call["args"] else None
                    for entry in last.get("m", []) if isinstance(last, dict) else []:
                        key = entry[0].get("s", "") if isinstance(entry[0], dict) else ""
                        if key.endswith("VALUE1") or key == "stream_filter_value1":
                            entry[1] = edge(r, entry[1])
        case["calls"] += extra_calls(r, spec)
    return cases


cc.frame_spec = frame_spec
cc.act_series = act_series
base.frame_spec = frame_spec
for _k in dir(cc):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(cc, _k))
globals()["frame_spec"] = frame_spec
globals()["act_series"] = act_series
globals()["generate_cases"] = generate_cases
