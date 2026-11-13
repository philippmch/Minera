"""Round CC: the pandas sequence filter domain with more datetime64 keys and
datetime arguments.

Loads scripts/pandas_sequence_filter_cb_domain.py and adds datetime64 case
and activity columns in s and ms (naive), us in Asia/Kolkata, ns in
America/New_York across the 2021 DST change, naive us around the epoch
(negative instants with fractions), and object columns of Timestamps
(naive, mixed with strings and None, or aware in two zones; their zone
names travel as date_timezone). On datetime
activity columns about half of the cases pass the activity values of the
calls as Python datetimes ({"w": ...}: naive, aware at the zone's offset,
aware in UTC, or mixed with strings within one list)."""
import datetime as _dt
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("pandas_sequence_filter_cb_domain",
                                               HERE / "pandas_sequence_filter_cb_domain.py")
cb = importlib.util.module_from_spec(_spec)
sys.modules["pandas_sequence_filter_cb_domain"] = cb
_spec.loader.exec_module(cb)
typed = cb.typed
base = cb.base
codec = base.codec

NEW_DT = {"dt64s": ("s", None), "dt64ms": ("ms", None), "dt64kol": ("us", "Asia/Kolkata"),
          "dt64ny": ("ns", "America/New_York"), "dt64epoch": ("us", None)}
OBJECT_KINDS = ["objdt", "objmix", "objzones"]
NEW_KINDS = list(NEW_DT) + OBJECT_KINDS
_code = cb._code
_cb_spec = cb.frame_spec


def frame_spec(r):
    spec = _cb_spec(r)
    if r.random() < 0.35:
        spec["case_kind"] = r.choice(NEW_KINDS)
    if r.random() < 0.5:
        spec["act_kind"] = r.choice(NEW_KINDS)
    spec["arg_mode"] = r.choice(["plain", "plain", "value", "value", "mixed"])
    return spec


def stamp(kind, c, minutes):
    """The pandas Timestamp of code c in a column of `kind`."""
    import pandas as pd
    if kind in cb.DT_KINDS:
        unit, zone = cb.DT_KINDS[kind]
        value = pd.Timestamp(2021, 1, c, c % 24) + pd.Timedelta(minutes=minutes * c)
        return value.tz_localize(zone) if zone else value
    if kind == "dt64s":
        return pd.Timestamp(2021, 2, min(c, 28), c % 24, 5, c) + pd.Timedelta(minutes=minutes * c)
    if kind == "dt64ms":
        return pd.Timestamp(2021, 2, min(c, 28), c % 24, 30, 0, 7000 * c) + pd.Timedelta(minutes=minutes * c)
    if kind == "dt64kol":
        return (pd.Timestamp(2021, 1, c, c % 24, 15) + pd.Timedelta(minutes=minutes * c)).tz_localize("Asia/Kolkata")
    if kind == "dt64ny":
        return (pd.Timestamp("2021-11-07 04:00", tz="UTC") + pd.Timedelta(minutes=40 * c + minutes * c)
                ).tz_convert("America/New_York")
    if kind == "dt64epoch":
        return pd.Timestamp(1969, 12, 31, 23, 59, 58) + pd.Timedelta(milliseconds=250 * c + 1000 * minutes * c)
    if kind == "objzones":
        value = pd.Timestamp(2021, 1, c, c % 24) + pd.Timedelta(minutes=minutes * c)
        return value.tz_localize("UTC" if c % 2 else "Europe/Berlin")
    return pd.Timestamp(2021, 1, c, c % 24) + pd.Timedelta(minutes=minutes * c)


def _series(spec_kind, codes, minutes):
    import pandas as pd
    if spec_kind in NEW_DT:
        unit, zone = NEW_DT[spec_kind]
        values = [pd.NaT if c is None else stamp(spec_kind, c, minutes) for c in codes]
        dtype = f"datetime64[{unit}, {zone}]" if zone else f"datetime64[{unit}]"
        return pd.Series(values, dtype=dtype)
    if spec_kind == "objmix":
        values = [None if c is None else (str(stamp("objdt", c, minutes)) if c % 3 == 0 else stamp("objdt", c, minutes))
                  for c in codes]
    else:
        values = [None if c is None else stamp(spec_kind, c, minutes) for c in codes]
    return pd.Series(values, dtype="object")


_cb_case = typed._case_series
_cb_act = typed._act_series


def case_series(spec, pd):
    if spec["case_kind"] in NEW_KINDS:
        return _series(spec["case_kind"], spec["cases"], 7)
    return _cb_case(spec, pd)


def act_series(spec, pd):
    if spec["act_kind"] in NEW_KINDS:
        return _series(spec["act_kind"], [_code(a) for a in spec["acts"]], 0)
    return _cb_act(spec, pd)


typed._case_series = case_series
typed._act_series = act_series

DATETIME_KINDS = set(cb.DT_KINDS) | set(NEW_KINDS)


def _temporal_arg(r, kind, text, mode):
    if text not in cb.ORDER or (mode == "mixed" and r.random() < 0.3):
        return {"s": text}
    value = stamp(kind, _code(text), 0).to_pydatetime()
    pick = r.random()
    if value.tzinfo is not None:
        if pick < 0.5:
            return codec.wall(value)
        if pick < 0.8:
            return codec.wall(value.astimezone(_dt.timezone.utc))
        return codec.wall(value.replace(tzinfo=None))
    if pick < 0.8 or mode != "mixed":
        return codec.wall(value)
    return codec.wall(value.replace(tzinfo=_dt.timezone.utc))


def transform(r, node, kind, mode, depth=0):
    (tag, value), = node.items()
    if tag == "s":
        return _temporal_arg(r, kind, value, mode)
    if tag in ("l", "t"):
        items = [transform(r, v, kind, mode, depth + 1) for v in value]
        return {tag: items}
    return node


_cb_generate = cb.generate_cases


def generate_cases(count, bank_seed):
    cases = _cb_generate(count, bank_seed)
    rng = random.Random(bank_seed ^ 0xCC)
    for case in cases:
        spec = case["spec"]
        r = random.Random(rng.randrange(1 << 30))
        kind, mode = spec.get("act_kind"), spec.get("arg_mode", "plain")
        if mode == "plain" or kind not in DATETIME_KINDS:
            continue
        for call in case["calls"]:
            if call["function"] in cb.VALUE_FUNCTIONS and call["args"] and "m" not in call["args"][0]:
                call["args"][0] = transform(r, call["args"][0], kind, mode)
    return cases


_table_json = base.tables.pandas_table_json


def pandas_table_json(df):
    """The table transport plus the zone name of each aware Timestamp in an
    object column (date_timezone), which pandas keeps per object and the
    plain transport only sends for datetime64 columns."""
    import pandas as pd
    out = _table_json(df)
    for i in range(len(df.columns)):
        if str(df.dtypes.iloc[i]) != "object":
            continue
        for row, value in zip(out["rows"], df.iloc[:, i].tolist()):
            cell = row[i]
            if isinstance(value, pd.Timestamp) and value.tz is not None and isinstance(cell, dict) \
                    and cell.get("kind") == "date" and not cell.get("null"):
                cell["date_timezone"] = str(value.tz)
    return out


base.tables.pandas_table_json = pandas_table_json
base.frame_spec = frame_spec
typed.frame_spec = frame_spec
cb.frame_spec = frame_spec
for _k in dir(cb):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(cb, _k))
globals()["frame_spec"] = frame_spec
globals()["generate_cases"] = generate_cases
