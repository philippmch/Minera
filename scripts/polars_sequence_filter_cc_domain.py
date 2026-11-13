"""Round CC: the Polars sequence filter domain with temporal arguments and
more activity dtypes.

Loads scripts/polars_sequence_filter_cb_domain.py and adds activity columns
of Datetime in ms (naive), us in Asia/Kolkata, ns in America/New_York across
the 2021 DST change, us in Europe/Berlin across both 2021 changes, naive us
around the epoch (negative instants with fractions), Date around the epoch,
and the narrow numeric dtypes Int8 to UInt64 and Float32 (with nulls). On
temporal activity columns about half of the cases pass the activity values
of the calls as Python datetimes ({"w": ...}: naive, aware at the zone's
offset, aware in UTC, or mixed within one list) or dates ({"date": ...});
on integer columns some values become floats, bools or nested lists."""
import datetime as _dt
import importlib.util
import random
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("polars_sequence_filter_cb_domain",
                                               HERE / "polars_sequence_filter_cb_domain.py")
cb = importlib.util.module_from_spec(_spec)
sys.modules["polars_sequence_filter_cb_domain"] = cb
_spec.loader.exec_module(cb)
typed = cb.typed
base = cb.base
codec = base.codec

NEW_KINDS = ["DatetimeMs", "DatetimeKolkata", "DatetimeNsNY", "DatetimeBerlinDst", "DatetimeEpoch", "DateEpoch",
             "Int8", "Int16", "Int32", "UInt16", "UInt32", "UInt64", "Float32"]
TEMPORAL = {"Datetime", "DatetimeUTC", "DatetimeNs", "Date", "DatetimeNsUTC", "DatetimeMsBerlin", "DatetimeMs",
            "DatetimeKolkata", "DatetimeNsNY", "DatetimeBerlinDst", "DatetimeEpoch", "DateEpoch"}
NUMERIC = {"Int64", "UInt8", "Int8", "Int16", "Int32", "UInt16", "UInt32", "UInt64", "Float32"}
_code = cb._code
_cb_spec = cb.frame_spec


def frame_spec(r):
    spec = _cb_spec(r)
    if r.random() < 0.5:
        spec["act_kind"] = r.choice(NEW_KINDS)
    spec["arg_mode"] = r.choice(["plain", "plain", "value", "value", "mixed"])
    return spec


def _zone(name):
    import zoneinfo
    return zoneinfo.ZoneInfo(name)


def act_value(kind, c):
    """The Python value of activity code c in an activity column of `kind`
    (a naive or aware datetime, a date, an int or a float)."""
    if c is None:
        return None
    if kind == "Datetime":
        return _dt.datetime(2021, 1, c, c % 24)
    if kind == "DatetimeUTC":
        return _dt.datetime(2021, 1, c, c % 24, tzinfo=_dt.timezone.utc)
    if kind in ("DatetimeNs", "DatetimeNsUTC"):
        return _dt.datetime(2021, 1, c, c % 24, tzinfo=_dt.timezone.utc if kind == "DatetimeNsUTC" else None)
    if kind == "Date":
        return _dt.date(2021, 1, c)
    if kind == "DatetimeMsBerlin":
        return _dt.datetime(2021, 3, 20 + min(c, 11), c % 24, 0, 0, 1000 * c, tzinfo=_zone("Europe/Berlin"))
    if kind == "DatetimeMs":
        return _dt.datetime(2021, 2, min(c, 28), c % 24, 30, 0, 7000 * c)
    if kind == "DatetimeKolkata":
        return _dt.datetime(2021, 1, c, c % 24, 15, tzinfo=_zone("Asia/Kolkata"))
    if kind == "DatetimeNsNY":
        utc = _dt.datetime(2021, 11, 7, 4, 0, tzinfo=_dt.timezone.utc) + _dt.timedelta(minutes=40 * c)
        return utc.astimezone(_zone("America/New_York"))
    if kind == "DatetimeBerlinDst":
        start = _dt.datetime(2021, 3, 28, 0, 0, tzinfo=_dt.timezone.utc) if c % 2 else \
            _dt.datetime(2021, 10, 31, 0, 0, tzinfo=_dt.timezone.utc)
        return (start + _dt.timedelta(minutes=25 * c)).astimezone(_zone("Europe/Berlin"))
    if kind == "DatetimeEpoch":
        return _dt.datetime(1969, 12, 31, 23, 59, 58) + _dt.timedelta(milliseconds=250 * c)
    if kind == "DateEpoch":
        return _dt.date(1969, 12, 26) + _dt.timedelta(days=c)
    if kind == "Float32":
        return float(c)
    return c


def _dtype(pl, kind):
    return {"DatetimeMs": pl.Datetime("ms"), "DatetimeKolkata": pl.Datetime("us", "Asia/Kolkata"),
            "DatetimeNsNY": pl.Datetime("ns", "America/New_York"),
            "DatetimeBerlinDst": pl.Datetime("us", "Europe/Berlin"), "DatetimeEpoch": pl.Datetime("us"),
            "DateEpoch": pl.Date}.get(kind) or getattr(pl, kind)


_cb_act = typed._act


def act_series(spec, pl, name):
    kind = spec["act_kind"]
    if kind not in NEW_KINDS:
        return _cb_act(spec, pl, name)
    values = [act_value(kind, _code(a)) for a in spec["acts"]]
    return pl.Series(name, values, dtype=_dtype(pl, kind))


typed._act = act_series


def _wall(value):
    return codec.wall(value)


def _temporal_arg(r, kind, text, mode):
    """A string activity argument as a temporal value of the column's kind."""
    if text not in cb.ORDER or (mode == "mixed" and r.random() < 0.3):
        return {"s": text}
    value = act_value(kind, _code(text))
    if isinstance(value, _dt.datetime):
        pick = r.random()
        if value.tzinfo is not None:
            if pick < 0.5:
                return _wall(value)
            if pick < 0.8:
                return _wall(value.astimezone(_dt.timezone.utc))
            return _wall(value.replace(tzinfo=None))
        if pick < 0.8 or mode != "mixed":
            return _wall(value)
        return _wall(value.replace(tzinfo=_dt.timezone.utc))
    if isinstance(value, _dt.date):
        if r.random() < 0.75:
            return {"date": value.isoformat()}
        return _wall(_dt.datetime(value.year, value.month, value.day))
    return {"s": text}


def _numeric_arg(r, kind, text, mode):
    if text not in cb.ORDER:
        return {"s": text}
    c = _code(text)
    pick = r.random()
    if mode == "mixed" and pick < 0.2:
        return {"s": text}
    if pick < 0.6:
        return {"i": str(c)}
    if pick < 0.85:
        return {"f": codec.fbits(float(c) + (0.5 if r.random() < 0.15 else 0.0))}
    if c in (1, 0):
        return {"b": bool(c)}
    return {"i": str(c)}


def transform(r, node, kind, mode, depth=0):
    (tag, value), = node.items()
    if tag == "s":
        if kind in TEMPORAL:
            return _temporal_arg(r, kind, value, mode)
        if kind in NUMERIC:
            return _numeric_arg(r, kind, value, mode)
        return node
    if tag in ("l", "t", "S"):
        items = [transform(r, v, kind, mode, depth + 1) for v in value]
        if mode == "mixed" and depth == 0 and tag != "S" and items and r.random() < 0.1:
            items[r.randrange(len(items))] = {"l": [items[0]]}
        if tag == "S":
            items = sorted(items, key=codec.text)
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
        if mode == "plain" or (kind not in TEMPORAL and kind not in NUMERIC):
            continue
        for call in case["calls"]:
            if call["function"] in cb.VALUE_FUNCTIONS and call["args"] and "m" not in call["args"][0]:
                call["args"][0] = transform(r, call["args"][0], kind, mode)
    return cases


_codec_decode = codec.decode


def decode(node):
    """The bank codec plus {"date": "YYYY-MM-DD"} as a datetime.date."""
    (tag, value), = node.items()
    if tag == "date":
        return _dt.date.fromisoformat(value)
    return _codec_decode(node)


codec.decode = decode
base.frame_spec = frame_spec
typed.frame_spec = frame_spec
cb.frame_spec = frame_spec
for _k in dir(cb):
    if not _k.startswith("__"):
        globals().setdefault(_k, getattr(cb, _k))
globals()["frame_spec"] = frame_spec
globals()["generate_cases"] = generate_cases
